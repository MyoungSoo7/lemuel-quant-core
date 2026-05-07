using JuMP, HiGHS, LinearAlgebra, Statistics

"""
    optimize_portfolio(returns; target_return=nothing, max_weight=1.0,
                       allow_short=false)

Markowitz mean-variance. Minimize portfolio variance subject to weights
summing to 1 and (optionally) achieving `target_return`. `returns` is an
n×k matrix (n observations, k assets).

Returns `(weights, expected_return, volatility)`.
"""
function optimize_portfolio(returns::AbstractMatrix;
                            target_return=nothing,
                            max_weight=1.0,
                            allow_short::Bool=false)
    n, k = size(returns)
    μ = vec(mean(returns; dims=1))
    Σ = cov(returns)

    model = Model(HiGHS.Optimizer)
    set_silent(model)
    if allow_short
        @variable(model, -max_weight <= w[1:k] <= max_weight)
    else
        @variable(model, 0 <= w[1:k] <= max_weight)
    end
    @constraint(model, sum(w) == 1)
    if target_return !== nothing
        @constraint(model, dot(μ, w) >= target_return)
    end
    @objective(model, Min, w' * Σ * w)
    optimize!(model)

    if termination_status(model) != MOI.OPTIMAL
        @warn "non-optimal status" status=termination_status(model)
    end
    wv = value.(w)
    er = dot(μ, wv)
    vol = sqrt(wv' * Σ * wv)
    return (weights=wv, expected_return=er, volatility=vol)
end

"""
    efficient_frontier(returns; n_points=20, allow_short=false, max_weight=1.0)

Sweep target_return between min and max asset returns; returns a vector of
(target, vol, weights) tuples.
"""
function efficient_frontier(returns::AbstractMatrix;
                            n_points::Int=20,
                            allow_short::Bool=false,
                            max_weight::Real=1.0)
    μ = vec(mean(returns; dims=1))
    rmin, rmax = minimum(μ), maximum(μ)
    targets = range(rmin, rmax; length=n_points)
    out = []
    for t in targets
        try
            r = optimize_portfolio(returns; target_return=t,
                                    allow_short, max_weight)
            push!(out, (target=t, vol=r.volatility, weights=r.weights))
        catch e
            @warn "frontier point failed" target=t err=e
        end
    end
    return out
end
